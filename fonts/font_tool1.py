import textwrap

# --- 1. DONNÉES DE LA POLICE (FORMAT C-ARRAY) ---
GLYPH_HEIGHT = 16  # Hauteur totale de la zone de données (2 pages de 8)
GLYPH_WIDTH = 10   # Largeur des pixels (colonnes)
BYTES_PER_GLYPH = 20
MAX_LINES_TO_RENDER = 15 # Hauteur utile confirmée (0 à 14)

font_data_str = """
0xFC, 0xFE, 0xFE, 0x06, 0x06, 0x06, 0x06, 0xFE, 0xFE, 0xFC,          0x3F, 0x7F, 0x7F, 0x60, 0x60, 0x60, 0x60, 0x7F, 0x7F, 0x3F,
0x00, 0x00, 0x18, 0x1C, 0xFE, 0xFE, 0xFE, 0x00, 0x00, 0x00,          0x00, 0x00, 0x00, 0x00, 0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x00,
0x1C, 0x1E, 0x0E, 0x06, 0x06, 0x06, 0x86, 0xFE, 0xFE, 0x7C,          0x20, 0x70, 0x78, 0x7C, 0x6E, 0x67, 0x63, 0x61, 0x60, 0x20,
0x04, 0x06, 0x06, 0x86, 0x86, 0x86, 0x86, 0xFE, 0xFE, 0x7C,          0x20, 0x60, 0x60, 0x61, 0x61, 0x61, 0x61, 0x7F, 0x7F, 0x3E,
0x80, 0xC0, 0xE0, 0x70, 0x38, 0x1C, 0x0E, 0xFE, 0xFE, 0xFE,          0x0F, 0x0F, 0x0F, 0x0C, 0x0C, 0x0C, 0x0C, 0x7F, 0x7F, 0x7F,
0xFC, 0xFE, 0xFE, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x04,          0x20, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x7F, 0x7F, 0x3F,
0xFC, 0xFE, 0xFE, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x04,          0x3F, 0x7F, 0x7F, 0x61, 0x61, 0x61, 0x61, 0x7F, 0x7F, 0x3F,
0x04, 0x06, 0x06, 0x06, 0x06, 0x86, 0xE6, 0xFE, 0x7E, 0x1C,          0x00, 0x00, 0x00, 0x00, 0x7C, 0x7F, 0x7F, 0x03, 0x00, 0x00,
0x7C, 0xFE, 0xFE, 0x86, 0x86, 0x86, 0x86, 0xFE, 0xFE, 0x7C,          0x3F, 0x7F, 0x7F, 0x61, 0x61, 0x61, 0x61, 0x7F, 0x7F, 0x3F,
0xFC, 0xFE, 0xFE, 0x86, 0x86, 0x86, 0x86, 0xFE, 0xFE, 0xFC,          0x20, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x7F, 0x7F, 0x3F,
0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,          0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00
"""

def parse_font_data(data_string, bytes_per_glyph=BYTES_PER_GLYPH):
    """
    Analyse la chaîne de données de police pour créer une liste de glyphes.
    """
    cleaned_data = data_string.replace(' ', '').replace('\n', ',').strip(',')
    try:
        byte_values = [int(val.strip(), 16) for val in cleaned_data.split(',') if val.strip()]
    except ValueError as e:
        print(f"Erreur de parsing: {e}")
        return []

    glyphs = [byte_values[i:i + bytes_per_glyph] 
              for i in range(0, len(byte_values), bytes_per_glyph)]
    return glyphs

def render_glyph_lines(glyph_bytes):
    """
    Calcule et retourne les 15 lignes de pixels pour un glyphe donné (Page-based, 10x16).
    """
    if len(glyph_bytes) != BYTES_PER_GLYPH:
        return ["Erreur de taille"] * MAX_LINES_TO_RENDER

    display_lines = [""] * GLYPH_HEIGHT

    bytes_page1 = glyph_bytes[:GLYPH_WIDTH]
    bytes_page2 = glyph_bytes[GLYPH_WIDTH:]

    # Parcourt les 10 colonnes
    for col in range(GLYPH_WIDTH):
        byte1 = bytes_page1[col] # Pixels Lignes 0-7
        byte2 = bytes_page2[col] # Pixels Lignes 8-15

        # Affichage Page 1 (Lignes 0-7)
        for row in range(8):
            is_pixel_on = (byte1 >> row) & 1
            display_lines[row] += "█" if is_pixel_on else " "
        
        # Affichage Page 2 (Lignes 8-15)
        for row in range(8):
            is_pixel_on = (byte2 >> row) & 1
            display_lines[row + 8] += "█" if is_pixel_on else " "

    # Retourne seulement les 15 lignes utilisées (0 à 14)
    return display_lines[:MAX_LINES_TO_RENDER]

def display_all_glyphs(glyphs):
    """
    Affiche tous les glyphes, groupés par 10.
    """
    LABELS = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.']
    GLYPHS_PER_ROW = 10 # Changement ici
    NUM_GLYPHS = len(glyphs)
    
    print(f"--- Visualisation de la police ({MAX_LINES_TO_RENDER}px H, {GLYPH_WIDTH}px L) - 10 Caractères par Ligne ---")
    
    # Itère sur les glyphes par lots de 10
    for i in range(0, NUM_GLYPHS, GLYPHS_PER_ROW):
        batch_glyphs = glyphs[i:i + GLYPHS_PER_ROW]
        batch_labels = LABELS[i:i + len(batch_glyphs)]
        
        # 1. Rendu des lignes pour chaque glyphe du lot
        rendered_batches = [render_glyph_lines(g) for g in batch_glyphs]
        
        # 2. Impression de l'en-tête (étiquettes)
        header_line = "  "
        separator_line = "  "
        
        # Largeur d'un caractère = 10 pixels + 2 espaces de séparation = 12
        for label, index_start in zip(batch_labels, range(i, i + len(batch_labels))):
            header_line += f"'{label}'(idx {index_start})".ljust(12) 
            separator_line += "----------  "
            
        print("\n" + header_line)
        print(separator_line)

        # 3. Concaténation et impression des lignes (0 à 14)
        for line_index in range(MAX_LINES_TO_RENDER):
            combined_line = "  "
            for rendered_glyph_lines in rendered_batches:
                # Ajoute la ligne et 2 espaces de séparation
                combined_line += rendered_glyph_lines[line_index] + "  " 
            print(combined_line)
            
        print("-" * (len(batch_glyphs) * 12 + 2)) # Ligne de séparation entre les lots

    print("--- Fin de la visualisation ---")

def generate_c_array_line(glyph_bytes, index=None):
    """
    Génère la ligne du tableau C pour un glyphe donné.
    """
    if len(glyph_bytes) != BYTES_PER_GLYPH:
        return "// Erreur: données du glyphe incomplètes"
        
    hex_values = [f"0x{b:02X}" for b in glyph_bytes]
    
    part1 = ", ".join(hex_values[:10])
    part2 = ", ".join(hex_values[10:])
    
    line = f"{{/*0x00, 0x00,*/ {part1}, /*0x00,*/ /*0x00, 0x00,*/ {part2} /*0x00*/}},"
    
    return line


# --- Exécution du script ---
if __name__ == "__main__":
    
    all_glyphs = parse_font_data(font_data_str)
    
    if all_glyphs:
        display_all_glyphs(all_glyphs)
        